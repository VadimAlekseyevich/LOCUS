#include "app/skin_controls.h"

#include <algorithm>
#include <cstdio>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <locale>
#include <sstream>
#include <string>

namespace locus::app {

bool GetSkinNumberRange(int page, int row, SkinNumberRange& range) noexcept {
    if (page == 0) {
        switch (row) {
            case 0: range = {200.0, 2000.0, 50.0, 10000.0}; return true;
            case 1: range = {1.0, 12.0, 0.25, 40.0}; return true;
            case 2: range = {1.0, 3.0, 1.0, 8.0}; return true;
            case 3: range = {0.5, 2.5, 0.1, 8.0}; return true;
            case 4: range = {0.0, 1.0, 0.0, 1.0}; return true;
            case 5: range = {3.0, 18.0, 1.0, 64.0}; return true;
            case 7: range = {0.0, 6.0, 0.0, 12.0}; return true;
            default: return false;
        }
    }
    if (page == 1) {
        switch (row) {
            case 0: range = {6.0, 22.0, 1.0, 80.0}; return true;
            case 1: range = {60.0, 320.0, kMinBaseSpeed, kMaxBaseSpeed}; return true;
            case 3: range = {0.5, 2.0, 0.1, 8.0}; return true;
            default: return false;
        }
    }
    if (page == 2 && row >= 0 && row < 4) {
        // RGBA has a hard representational limit: 8-bit integer channels.
        range = {0.0, 255.0, 0.0, 255.0};
        return true;
    }
    return false;
}

double GetSkinNumber(const Skin& skin, int page, int row,
                     int selectedColor) noexcept {
    if (page == 0) {
        switch (row) {
            case 0: return skin.lookaheadMs;
            case 1: return skin.pathWidth;
            case 2: return skin.outlineScale;
            case 3: return skin.fadePower;
            case 4: return skin.pathOpacity;
            case 5: return skin.junctionRadius;
            case 7: return skin.reverseCapScale;
        }
    } else if (page == 1) {
        switch (row) {
            case 0: return skin.playerRadius;
            case 1: return skin.baseSpeedPxPerSecond;
            case 3: return skin.hitFlashScale;
        }
    } else if (page == 2 && selectedColor >= 0 &&
               selectedColor < kSkinColorCount) {
        const Color color = SkinColor(skin, selectedColor);
        switch (row) {
            case 0: return color.r;
            case 1: return color.g;
            case 2: return color.b;
            case 3: return color.a;
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
        // Sliders are rounded to two decimals; typing retains more precision.
        if (fromSlider) value = std::round(value * 100.0) / 100.0;
        const float number = static_cast<float>(value);
        if (page == 0) {
            switch (row) {
                case 0: skin.lookaheadMs = number; break;
                case 1: skin.pathWidth = number; break;
                case 2: skin.outlineScale = number; break;
                case 3: skin.fadePower = number; break;
                case 4: skin.pathOpacity = number; break;
                case 5: skin.junctionRadius = number; break;
                case 7: skin.reverseCapScale = number; break;
                default: return false;
            }
        } else if (page == 1) {
            switch (row) {
                case 0: skin.playerRadius = number; break;
                case 1: skin.baseSpeedPxPerSecond = number; break;
                case 3: skin.hitFlashScale = number; break;
                default: return false;
            }
        } else return false;
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
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << std::setprecision(8) << value;
    return stream.str();
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
