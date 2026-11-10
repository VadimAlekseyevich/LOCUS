#include "app/skin_controls.h"

#include <algorithm>
#include <cstdio>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iterator>
#include <locale>
#include <sstream>
#include <string>

namespace locus::app {

namespace {
// The display order is the array order. Adding a new presentation control
// requires one entry rather than synchronized row indices across UI, range,
// getter, setter and boolean dispatch.
constexpr SkinSetting kPathSettings[] = {
    {"PREVIEW TIME (MS)", {200, 2000, 50, 10000}, &Skin::lookaheadMs, nullptr, true},
    {"PATH WIDTH",       {1, 12, 0.25, 40},       &Skin::pathWidth},
    {"OUTLINE SCALE",    {1, 3, 1, 8},           &Skin::outlineScale},
    {"FADE CURVE",       {0.5, 2.5, 0.1, 8},     &Skin::fadePower},
    {"PATH OPACITY",     {0, 1, 0, 1},           &Skin::pathOpacity},
    {"TURN SIZE",        {3, 18, 1, 64},         &Skin::junctionRadius},
    {"TURN GLOW",        {}, nullptr, &Skin::junctionGlow},
    {"REVERSE CAP SIZE", {0, 6, 0, 12},          &Skin::reverseCapScale}
};
constexpr SkinSetting kPlayerSettings[] = {
    {"PLAYER SIZE",      {6, 22, 1, 80},            &Skin::playerRadius},
    {"SPEED (PX/S)",     {60, 320, kMinBaseSpeed, kMaxBaseSpeed},
                                                    &Skin::baseSpeedPxPerSecond},
    {"PLAYER GLOW",      {}, nullptr, &Skin::playerGlow},
    {"HIT FLASH SIZE",   {0.5, 2, 0.1, 8},          &Skin::hitFlashScale},
    {"SHOW HUD",         {}, nullptr, &Skin::showHud},
    {"CAMERA LAG (MS)",  {0, 500, 0, 1500},         &Skin::cameraLagMs, nullptr, true}
};
}

const SkinSetting* GetSkinSetting(int page, int row) noexcept {
    if (row < 0) return nullptr;
    if (page == 0 && row < static_cast<int>(std::size(kPathSettings)))
        return &kPathSettings[row];
    if (page == 1 && row < static_cast<int>(std::size(kPlayerSettings)))
        return &kPlayerSettings[row];
    return nullptr;
}

int SkinSettingCount(int page) noexcept {
    return page == 0 ? static_cast<int>(std::size(kPathSettings))
         : page == 1 ? static_cast<int>(std::size(kPlayerSettings)) : 0;
}

bool GetSkinToggle(const Skin& skin, int page, int row) noexcept {
    const SkinSetting* setting = GetSkinSetting(page, row);
    return setting && setting->toggle ? skin.*(setting->toggle) : false;
}

bool FlipSkinToggle(Skin& skin, int page, int row) noexcept {
    const SkinSetting* setting = GetSkinSetting(page, row);
    if (!setting || !setting->toggle) return false;
    skin.*(setting->toggle) = ! (skin.*(setting->toggle));
    return true;
}

bool GetSkinNumberRange(int page, int row, SkinNumberRange& range) noexcept {
    if (const SkinSetting* setting = GetSkinSetting(page, row)) {
        if (!setting->number) return false;
        range = setting->numberRange;
        return true;
    }
    if (page == 2 && row >= 0 && row < 4) {
        range = {0, 255, 0, 255};
        return true;
    }
    return false;
}

double GetSkinNumber(const Skin& skin, int page, int row,
                     int selectedColor) noexcept {
    if (const SkinSetting* setting = GetSkinSetting(page, row)) {
        if (setting->number) return skin.*(setting->number);
        return 0.0;
    }
    if (page == 2 && selectedColor >= 0 &&
        selectedColor < kSkinColorCount) {
        const Color color = SkinColor(skin, selectedColor);
        switch (row) {
            case 0: return color.r;
            case 1: return color.g;
            case 2: return color.b;
            case 3: return color.a;
            default: break;
        }
    }
    return 0.0;
}

bool SetSkinNumber(Skin& skin, int page, int row, int selectedColor,
                   double value, bool fromSlider, std::string& error) {
    SkinNumberRange range;
    if (!GetSkinNumberRange(page, row, range) || !std::isfinite(value)) {
        error = "Invalid numeric skin value.";
        return false;
    }
    if (fromSlider)
        value = std::clamp(value, range.sliderMin, range.sliderMax);
    else if (value < range.inputMin || value > range.inputMax) {
        error = "INPUT RANGE: " + FormatSkinNumber(range.inputMin) + " .. " +
                FormatSkinNumber(range.inputMax);
        return false;
    }

    if (page == 2) {
        if (selectedColor < 0 || selectedColor >= kSkinColorCount ||
            (!fromSlider && std::floor(value) != value)) {
            error = "RGBA channels must be whole numbers from 0 to 255.";
            return false;
        }
        const auto byte = static_cast<unsigned char>(std::lround(value));
        Color& color = SkinColor(skin, selectedColor);
        switch (row) {
            case 0: color.r = byte; break;
            case 1: color.g = byte; break;
            case 2: color.b = byte; break;
            case 3: color.a = byte; break;
            default: return false;
        }
    } else {
        const SkinSetting* setting = GetSkinSetting(page, row);
        if (!setting || !setting->number) {
            error = "Invalid numeric skin value.";
            return false;
        }
        // Keep pixel/size fields to hundredths; durations to whole ms.
        value = setting->wholeMilliseconds
            ? std::round(value) : std::round(value * 100.0) / 100.0;
        skin.*(setting->number) = static_cast<float>(value);
    }
    error.clear();
    return true;
}

bool ParseSkinNumber(std::string_view text, double& result) noexcept {
    if (text.empty() || text.size() > 32) return false;
    std::string normalized(text);
    for (char& c : normalized)
        if (c == ',') c = '.';
    errno = 0;
    char* tail = nullptr;
    const double parsed = std::strtod(normalized.c_str(), &tail);
    if (errno == ERANGE || tail == normalized.c_str() ||
        *tail != '\0' || !std::isfinite(parsed)) return false;
    result = parsed;
    return true;
}

std::string FormatSkinNumber(double value) {
    if (!std::isfinite(value)) return "INVALID";
    // Fixed decimals avoid exposing binary-float noise (1.8200001). Strip
    // insignificant zeroes so integer settings are shown as "800", not
    // "800.00"; never display negative zero after rounding.
    const double rounded = std::round(value * 100.0) / 100.0;
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << std::fixed << std::setprecision(2)
           << (rounded == 0.0 ? 0.0 : rounded);
    std::string result = stream.str();
    while (!result.empty() && result.back() == '0') result.pop_back();
    if (!result.empty() && result.back() == '.') result.pop_back();
    return result;
}

SkinHsv SkinColorToHsv(Color color) noexcept {
    const float r = color.r / 255.0f;
    const float g = color.g / 255.0f;
    const float b = color.b / 255.0f;
    const float high = std::max({r,g,b}), low = std::min({r,g,b});
    const float diff = high - low;
    SkinHsv hsv;
    hsv.v = high;
    hsv.s = high > 0.0f ? diff / high : 0.0f;
    if (diff <= 0.0f) return hsv;
    if (high == r) hsv.h = (g - b) / diff;
    else if (high == g) hsv.h = 2.0f + (b - r) / diff;
    else hsv.h = 4.0f + (r - g) / diff;
    hsv.h /= 6.0f;
    if (hsv.h < 0.0f) hsv.h += 1.0f;
    return hsv;
}

Color SkinColorFromHsv(SkinHsv hsv, unsigned char alpha) noexcept {
    const float h = std::isfinite(hsv.h) ? hsv.h - std::floor(hsv.h) : 0.0f;
    const float s = std::clamp(std::isfinite(hsv.s) ? hsv.s : 0.0f, 0.0f, 1.0f);
    const float v = std::clamp(std::isfinite(hsv.v) ? hsv.v : 0.0f, 0.0f, 1.0f);
    const float c = v * s;
    const float hue = h * 6.0f;
    const float x = c * (1.0f - std::abs(std::fmod(hue, 2.0f) - 1.0f));
    const float m = v - c;
    float r=0.0f,g=0.0f,b=0.0f;
    if (hue < 1.0f) { r=c; g=x; }
    else if (hue < 2.0f) { r=x; g=c; }
    else if (hue < 3.0f) { g=c; b=x; }
    else if (hue < 4.0f) { g=x; b=c; }
    else if (hue < 5.0f) { r=x; b=c; }
    else { r=c; b=x; }
    const auto channel = [m](float f) {
        return static_cast<unsigned char>(
            std::lround(std::clamp(f+m,0.0f,1.0f)*255.0f));
    };
    return {channel(r),channel(g),channel(b),alpha};
}

std::string FormatSkinHex(Color color) {
    char formatted[10]{};
    std::snprintf(formatted, sizeof(formatted), "#%02X%02X%02X%02X",
                  color.r, color.g, color.b, color.a);
    return formatted;
}

bool ParseSkinHex(std::string_view text, Color old, Color& result) noexcept {
    if (!text.empty() && text.front() == '#') text.remove_prefix(1);
    if (text.size() != 6 && text.size() != 8) return false;
    const auto hex = [](char c) -> int {
        if (c >= '0' && c <= '9') return c-'0';
        if (c >= 'a' && c <= 'f') return c-'a'+10;
        if (c >= 'A' && c <= 'F') return c-'A'+10;
        return -1;
    };
    unsigned char parsed[4]{};
    for (std::size_t i=0; i<text.size()/2; ++i) {
        const int a=hex(text[i*2]), b=hex(text[i*2+1]);
        if (a<0 || b<0) return false;
        parsed[i]=static_cast<unsigned char>((a<<4)|b);
    }
    result={parsed[0],parsed[1],parsed[2],
            text.size()==8 ? parsed[3] : old.a};
    return true;
}

} // namespace locus::app
